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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <inttypes.h>
#include <getopt.h>
#include <signal.h>

#include <rte_eal.h>
#include <rte_config.h>
#include <rte_cycles.h>
#include <rte_eal_memconfig.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_string_fns.h>
#include <rte_ivshmem.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>

#include "../include/common.h"

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT 128
#define RTE_TEST_TX_DESC_DEFAULT 512
static uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
static uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;

#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */

/* mask of enabled ports */
static uint32_t l2fwd_ivshmem_enabled_port_mask = 0;

static struct ether_addr l2fwd_ivshmem_ports_eth_addr[RTE_MAX_ETHPORTS];

#define NB_MBUF   8192

#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT 16
struct lcore_queue_conf {
	unsigned n_rx_port;
	unsigned rx_port_list[MAX_RX_QUEUE_PER_LCORE];
	struct vm_port_param * port_param[MAX_RX_QUEUE_PER_LCORE];
	struct mbuf_table tx_mbufs[RTE_MAX_ETHPORTS];
	struct mbuf_table rx_mbufs[RTE_MAX_ETHPORTS];
} __rte_cache_aligned;
static struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];

static const struct rte_eth_conf port_conf = {
	.rxmode = {
		.split_hdr_size = 0,
		.header_split   = 0, /**< Header Split disabled */
		.hw_ip_checksum = 0, /**< IP checksum offload disabled */
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		.jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
		.hw_strip_crc   = 0, /**< CRC stripped by hardware */
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};

#define METADATA_NAME "l2fwd_ivshmem"
#define CMDLINE_OPT_FWD_CONF "fwd-conf"

#define QEMU_CMD_FMT "/tmp/ivshmem_qemu_cmdline_%s"

struct port_statistics port_statistics[RTE_MAX_ETHPORTS];

struct rte_mempool * l2fwd_ivshmem_pktmbuf_pool = NULL;

/* Print out statistics on packets dropped */
static void
print_stats(void)
{
	uint64_t total_packets_dropped, total_packets_tx, total_packets_rx;
	uint64_t total_vm_packets_dropped, total_vm_packets_tx, total_vm_packets_rx;
	unsigned portid;

	total_packets_dropped = 0;
	total_packets_tx = 0;
	total_packets_rx = 0;
	total_vm_packets_tx = 0;
	total_vm_packets_rx = 0;

	const char clr[] = { 27, '[', '2', 'J', '\0' };
	const char topLeft[] = { 27, '[', '1', ';', '1', 'H','\0' };

		/* Clear screen and move to top left */
	printf("%s%s", clr, topLeft);

	printf("\nPort statistics ====================================");

	for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++) {
		/* skip disabled ports */
		if ((l2fwd_ivshmem_enabled_port_mask & (1 << portid)) == 0)
			continue;
		printf("\nStatistics for port %u ------------------------------"
			   "\nPackets sent: %24"PRIu64
			   "\nPackets received: %20"PRIu64
			   "\nPackets dropped: %21"PRIu64,
			   portid,
			   port_statistics[portid].tx,
			   port_statistics[portid].rx,
			   port_statistics[portid].dropped);

		total_packets_dropped += port_statistics[portid].dropped;
		total_packets_tx += port_statistics[portid].tx;
		total_packets_rx += port_statistics[portid].rx;
	}

	printf("\nVM statistics ======================================");
	for (portid = 0; portid < ctrl->nb_ports; portid++) {
		printf("\nStatistics for port %u ------------------------------"
			   "\nPackets sent: %24"PRIu64
			   "\nPackets received: %20"PRIu64,
			   portid,
			   ctrl->vm_ports[portid].stats.tx,
			   ctrl->vm_ports[portid].stats.rx);

		total_vm_packets_dropped += ctrl->vm_ports[portid].stats.dropped;
		total_vm_packets_tx += ctrl->vm_ports[portid].stats.tx;
		total_vm_packets_rx += ctrl->vm_ports[portid].stats.rx;
	}
	printf("\nAggregate statistics ==============================="
			   "\nTotal packets sent: %18"PRIu64
			   "\nTotal packets received: %14"PRIu64
			   "\nTotal packets dropped: %15"PRIu64
			   "\nTotal VM packets sent: %15"PRIu64
			   "\nTotal VM packets received: %11"PRIu64,
			   total_packets_tx,
			   total_packets_rx,
			   total_packets_dropped,
			   total_vm_packets_tx,
			   total_vm_packets_rx);
	printf("\n====================================================\n");
}

static int
print_to_file(const char *cmdline, const char *config_name)
{
	FILE *file;
	char path[PATH_MAX];

	snprintf(path, sizeof(path), QEMU_CMD_FMT, config_name);
	file = fopen(path, "w");
	if (file == NULL) {
		RTE_LOG(ERR, L2FWD_IVSHMEM, "Could not open '%s' \n", path);
		return -1;
	}

	RTE_LOG(DEBUG, L2FWD_IVSHMEM, "QEMU command line for config '%s': %s \n",
			config_name, cmdline);

	fprintf(file, "%s\n", cmdline);
	fclose(file);
	return 0;
}

static int
generate_ivshmem_cmdline(const char *config_name)
{
	char cmdline[PATH_MAX];
	if (rte_ivshmem_metadata_cmdline_generate(cmdline, sizeof(cmdline),
			config_name) < 0)
		return -1;

	if (print_to_file(cmdline, config_name) < 0)
		return -1;

	rte_ivshmem_metadata_dump(stdout, config_name);
	return 0;
}

/* display usage */
static void
l2fwd_ivshmem_usage(const char *prgname)
{
	printf("%s [EAL options] -- -p PORTMASK [-q NQ -T PERIOD]\n"
		   "  -p PORTMASK: hexadecimal bitmask of ports to configure\n"
		   "  -q NQ: number of queue (=ports) per lcore (default is 1)\n"
		   "  -T PERIOD: statistics will be refreshed each PERIOD seconds "
		       "(0 to disable, 10 default, 86400 maximum)\n",
	       prgname);
}

static unsigned int
l2fwd_ivshmem_parse_nqueue(const char *q_arg)
{
	char *end = NULL;
	unsigned long n;

	/* parse hexadecimal string */
	n = strtoul(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;
	if (n == 0)
		return 0;
	if (n >= MAX_RX_QUEUE_PER_LCORE)
		return 0;

	return n;
}

static int
l2fwd_ivshmem_parse_portmask(const char *portmask)
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
l2fwd_ivshmem_parse_timer_period(const char *q_arg)
{
	char *end = NULL;
	int n;

	/* parse number string */
	n = strtol(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;
	if (n >= MAX_TIMER_PERIOD)
		return -1;

	return n;
}

/* Parse the argument given in the command line of the application */
static int
l2fwd_ivshmem_parse_args(int argc, char **argv)
{
	int opt, ret;
	char **argvopt;
	int option_index;
	char *prgname = argv[0];
	static struct option lgopts[] = {
			{CMDLINE_OPT_FWD_CONF, 1, 0, 0},
		{NULL, 0, 0, 0}
	};

	argvopt = argv;

	while ((opt = getopt_long(argc, argvopt, "q:p:T:",
				  lgopts, &option_index)) != EOF) {

		switch (opt) {
		/* portmask */
		case 'p':
			l2fwd_ivshmem_enabled_port_mask = l2fwd_ivshmem_parse_portmask(optarg);
			if (l2fwd_ivshmem_enabled_port_mask == 0) {
				printf("invalid portmask\n");
				l2fwd_ivshmem_usage(prgname);
				return -1;
			}
			break;

		/* nqueue */
		case 'q':
			l2fwd_ivshmem_rx_queue_per_lcore = l2fwd_ivshmem_parse_nqueue(optarg);
			if (l2fwd_ivshmem_rx_queue_per_lcore == 0) {
				printf("invalid queue number\n");
				l2fwd_ivshmem_usage(prgname);
				return -1;
			}
			break;

		/* timer period */
		case 'T':
			timer_period = l2fwd_ivshmem_parse_timer_period(optarg) * 1000 * TIMER_MILLISECOND;
			if (timer_period < 0) {
				printf("invalid timer period\n");
				l2fwd_ivshmem_usage(prgname);
				return -1;
			}
			break;

		/* long options */
		case 0:
			l2fwd_ivshmem_usage(prgname);
			return -1;

		default:
			l2fwd_ivshmem_usage(prgname);
			return -1;
		}
	}

	if (optind >= 0)
		argv[optind-1] = prgname;

	ret = optind-1;
	optind = 0; /* reset getopt lib */
	return ret;
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

/* Send the burst of packets on an output interface */
static int
l2fwd_ivshmem_send_burst(struct lcore_queue_conf *qconf, unsigned n, uint8_t port)
{
	struct rte_mbuf **m_table;
	unsigned ret;
	unsigned queueid =0;

	m_table = (struct rte_mbuf **)qconf->tx_mbufs[port].m_table;

	ret = rte_eth_tx_burst(port, (uint16_t) queueid, m_table, (uint16_t) n);
	port_statistics[port].tx += ret;
	if (unlikely(ret < n)) {
		port_statistics[port].dropped += (n - ret);
		do {
			rte_pktmbuf_free(m_table[ret]);
		} while (++ret < n);
	}

	return 0;
}

/* Enqueue packets for TX and prepare them to be sent on the network */
static int
l2fwd_ivshmem_send_packet(struct rte_mbuf *m, uint8_t port)
{
	unsigned lcore_id, len;
	struct lcore_queue_conf *qconf;

	lcore_id = rte_lcore_id();

	qconf = &lcore_queue_conf[lcore_id];
	len = qconf->tx_mbufs[port].len;
	qconf->tx_mbufs[port].m_table[len] = m;
	len++;

	/* enough pkts to be sent */
	if (unlikely(len == MAX_PKT_BURST)) {
		l2fwd_ivshmem_send_burst(qconf, MAX_PKT_BURST, port);
		len = 0;
	}

	qconf->tx_mbufs[port].len = len;
	return 0;
}

static int
l2fwd_ivshmem_receive_burst(struct lcore_queue_conf *qconf, unsigned portid,
		unsigned vm_port)
{
	struct rte_mbuf ** m;
	struct rte_ring * rx;
	unsigned len, pkt_idx;

	m = qconf->rx_mbufs[portid].m_table;
	len = qconf->rx_mbufs[portid].len;
	rx = qconf->port_param[vm_port]->rx_ring;

	/* if enqueueing failed, ring is probably full, so drop the packets */
	if (rte_ring_enqueue_bulk(rx, (void**) m, len) < 0) {
		port_statistics[portid].dropped += len;

		pkt_idx = 0;
		do {
			rte_pktmbuf_free(m[pkt_idx]);
		} while (++pkt_idx < len);
	}
	else
		/* increment rx stats by however many packets we managed to receive */
		port_statistics[portid].rx += len;

	return 0;
}

/* Enqueue packets for RX and prepare them to be sent to VM */
static int
l2fwd_ivshmem_receive_packets(struct rte_mbuf ** m, unsigned n, unsigned portid,
		unsigned vm_port)
{
	unsigned lcore_id, len, pkt_idx;
	struct lcore_queue_conf *qconf;

	lcore_id = rte_lcore_id();

	qconf = &lcore_queue_conf[lcore_id];

	len = qconf->rx_mbufs[portid].len;
	pkt_idx = 0;

	/* enqueue packets */
	while (pkt_idx < n && len < MAX_PKT_BURST * 2) {
		qconf->rx_mbufs[portid].m_table[len++] = m[pkt_idx++];
	}

	/* increment queue len by however many packets we managed to receive */
	qconf->rx_mbufs[portid].len += pkt_idx;

	/* drop the unreceived packets */
	if (unlikely(pkt_idx < n)) {
		port_statistics[portid].dropped += n - pkt_idx;
		do {
			rte_pktmbuf_free(m[pkt_idx]);
		} while (++pkt_idx < n);
	}

	/* drain the queue halfway through the maximum capacity */
	if (unlikely(qconf->rx_mbufs[portid].len >= MAX_PKT_BURST))
		l2fwd_ivshmem_receive_burst(qconf, portid, vm_port);

	return 0;
}

/* loop for host forwarding mode.
 * the data flow is as follows:
 *  1) get packets from TX queue and send it out from a given port
 *  2) RX packets from given port and enqueue them on RX ring
 *  3) dequeue packets from TX ring and put them on TX queue for a given port
 */
static void
fwd_loop(void)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST * 2];
	struct rte_mbuf *m;
	unsigned lcore_id;
	uint64_t prev_tsc, diff_tsc, cur_tsc, timer_tsc;
	unsigned i, j, portid, nb_rx;
	struct lcore_queue_conf *qconf;
	struct rte_ring *tx;
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S * BURST_TX_DRAIN_US;

	prev_tsc = 0;
	timer_tsc = 0;

	lcore_id = rte_lcore_id();
	qconf = &lcore_queue_conf[lcore_id];

	if (qconf->n_rx_port == 0) {
		RTE_LOG(INFO, L2FWD_IVSHMEM, "lcore %u has nothing to do\n", lcore_id);
		return;
	}

	RTE_LOG(INFO, L2FWD_IVSHMEM, "entering main loop on lcore %u\n", lcore_id);

	for (i = 0; i < qconf->n_rx_port; i++) {

		portid = qconf->rx_port_list[i];
		RTE_LOG(INFO, L2FWD_IVSHMEM, " -- lcoreid=%u portid=%u\n", lcore_id,
			portid);
	}

	while (ctrl->state == STATE_FWD) {

		cur_tsc = rte_rdtsc();

		/*
		 * Burst queue drain
		 */
		diff_tsc = cur_tsc - prev_tsc;
		if (unlikely(diff_tsc > drain_tsc)) {

			/*
			 * TX
			 */
			for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++) {
				if (qconf->tx_mbufs[portid].len == 0)
					continue;
				l2fwd_ivshmem_send_burst(qconf,
						 qconf->tx_mbufs[portid].len,
						 (uint8_t) portid);
				qconf->tx_mbufs[portid].len = 0;
			}

			/*
			 * RX
			 */
			for (i = 0; i < qconf->n_rx_port; i++) {
				portid = qconf->rx_port_list[i];
				if (qconf->rx_mbufs[portid].len == 0)
					continue;
				l2fwd_ivshmem_receive_burst(qconf, portid, i);
				qconf->rx_mbufs[portid].len = 0;
			}

			/* if timer is enabled */
			if (timer_period > 0) {

				/* advance the timer */
				timer_tsc += diff_tsc;

				/* if timer has reached its timeout */
				if (unlikely(timer_tsc >= (uint64_t) timer_period)) {

					/* do this only on master core */
					if (lcore_id == rte_get_master_lcore()) {
						print_stats();
						/* reset the timer */
						timer_tsc = 0;
					}
				}
			}

			prev_tsc = cur_tsc;
		}

		/*
		 * packet RX and forwarding
		 */
		for (i = 0; i < qconf->n_rx_port; i++) {

			/* RX packets from port and put them on RX ring */
			portid = qconf->rx_port_list[i];
			nb_rx = rte_eth_rx_burst((uint8_t) portid, 0,
						 pkts_burst, MAX_PKT_BURST);

			if (nb_rx != 0)
				l2fwd_ivshmem_receive_packets(pkts_burst, nb_rx, portid, i);

			/* dequeue packets from TX ring and send them to TX queue */
			tx = qconf->port_param[i]->tx_ring;

			nb_rx = rte_ring_count(tx);

			nb_rx = RTE_MIN(nb_rx, (unsigned) MAX_PKT_BURST);

			if (nb_rx == 0)
				continue;

			/* should not happen */
			if (unlikely(rte_ring_dequeue_bulk(tx, (void**) pkts_burst, nb_rx) < 0)) {
				ctrl->state = STATE_FAIL;
				return;
			}

			for (j = 0; j < nb_rx; j++) {
				m = pkts_burst[j];
				l2fwd_ivshmem_send_packet(m, portid);
			}
		}
	}
}

static int
l2fwd_ivshmem_launch_one_lcore(__attribute__((unused)) void *dummy)
{
	fwd_loop();
	return 0;
}

int main(int argc, char **argv)
{
	char name[RTE_RING_NAMESIZE];
	struct rte_ring *r;
	struct lcore_queue_conf *qconf;
	struct rte_eth_dev_info dev_info;
	uint8_t portid, port_nr;
	uint8_t nb_ports, nb_ports_available;
	uint8_t nb_ports_in_mask;
	int ret;
	unsigned lcore_id, rx_lcore_id;

	/* init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
	argc -= ret;
	argv += ret;

	/* parse application arguments (after the EAL ones) */
	ret = l2fwd_ivshmem_parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid l2fwd-ivshmem arguments\n");

	/* create a shared mbuf pool */
	l2fwd_ivshmem_pktmbuf_pool =
		rte_pktmbuf_pool_create(MBUF_MP_NAME, NB_MBUF, 32,
			0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (l2fwd_ivshmem_pktmbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");

	nb_ports = rte_eth_dev_count();
	if (nb_ports == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

	if (nb_ports > RTE_MAX_ETHPORTS)
		nb_ports = RTE_MAX_ETHPORTS;

	/*
	 * reserve memzone to communicate with VMs - we cannot use rte_malloc here
	 * because while it is technically possible, it is a very bad idea to share
	 * the heap between two primary processes.
	 */
	ctrl_mz = rte_memzone_reserve(CTRL_MZ_NAME, sizeof(struct ivshmem_ctrl),
			SOCKET_ID_ANY, 0);
	if (ctrl_mz == NULL)
		rte_exit(EXIT_FAILURE, "Cannot reserve control memzone\n");
	ctrl = (struct ivshmem_ctrl*) ctrl_mz->addr;

	memset(ctrl, 0, sizeof(struct ivshmem_ctrl));

	/*
	 * Each port is assigned an output port.
	 */
	nb_ports_in_mask = 0;
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if ((l2fwd_ivshmem_enabled_port_mask & (1 << portid)) == 0)
			continue;
		if (portid % 2) {
			ctrl->vm_ports[nb_ports_in_mask].dst = &ctrl->vm_ports[nb_ports_in_mask-1];
			ctrl->vm_ports[nb_ports_in_mask-1].dst = &ctrl->vm_ports[nb_ports_in_mask];
		}

		nb_ports_in_mask++;

		rte_eth_dev_info_get(portid, &dev_info);
	}
	if (nb_ports_in_mask % 2) {
		printf("Notice: odd number of ports in portmask.\n");
		ctrl->vm_ports[nb_ports_in_mask-1].dst =
				&ctrl->vm_ports[nb_ports_in_mask-1];
	}

	rx_lcore_id = 0;
	qconf = NULL;

	printf("Initializing ports configuration...\n");

	nb_ports_available = nb_ports;

	/* Initialise each port */
	for (portid = 0; portid < nb_ports; portid++) {

		/* skip ports that are not enabled */
		if ((l2fwd_ivshmem_enabled_port_mask & (1 << portid)) == 0) {
			printf("Skipping disabled port %u\n", (unsigned) portid);
			nb_ports_available--;
			continue;
		}

		/* init port */
		printf("Initializing port %u... ", (unsigned) portid);
		fflush(stdout);
		ret = rte_eth_dev_configure(portid, 1, 1, &port_conf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n",
				  ret, (unsigned) portid);

		rte_eth_macaddr_get(portid,&l2fwd_ivshmem_ports_eth_addr[portid]);

		/* init one RX queue */
		fflush(stdout);
		ret = rte_eth_rx_queue_setup(portid, 0, nb_rxd,
						 rte_eth_dev_socket_id(portid),
						 NULL,
						 l2fwd_ivshmem_pktmbuf_pool);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u\n",
				  ret, (unsigned) portid);

		/* init one TX queue on each port */
		fflush(stdout);
		ret = rte_eth_tx_queue_setup(portid, 0, nb_txd,
				rte_eth_dev_socket_id(portid),
				NULL);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup:err=%d, port=%u\n",
				ret, (unsigned) portid);

		/* Start device */
		ret = rte_eth_dev_start(portid);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n",
				  ret, (unsigned) portid);

		printf("done: \n");

		rte_eth_promiscuous_enable(portid);

		printf("Port %u, MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n\n",
				(unsigned) portid,
				l2fwd_ivshmem_ports_eth_addr[portid].addr_bytes[0],
				l2fwd_ivshmem_ports_eth_addr[portid].addr_bytes[1],
				l2fwd_ivshmem_ports_eth_addr[portid].addr_bytes[2],
				l2fwd_ivshmem_ports_eth_addr[portid].addr_bytes[3],
				l2fwd_ivshmem_ports_eth_addr[portid].addr_bytes[4],
				l2fwd_ivshmem_ports_eth_addr[portid].addr_bytes[5]);

		/* initialize port stats */
		memset(&port_statistics, 0, sizeof(port_statistics));
	}

	if (!nb_ports_available) {
		rte_exit(EXIT_FAILURE,
			"All available ports are disabled. Please set portmask.\n");
	}
	port_nr = 0;

	/* Initialize the port/queue configuration of each logical core */
	for (portid = 0; portid < nb_ports; portid++) {
		if ((l2fwd_ivshmem_enabled_port_mask & (1 << portid)) == 0)
			continue;

		/* get the lcore_id for this port */
		while (rte_lcore_is_enabled(rx_lcore_id) == 0 ||
			   lcore_queue_conf[rx_lcore_id].n_rx_port ==
					   l2fwd_ivshmem_rx_queue_per_lcore) {
			rx_lcore_id++;
			if (rx_lcore_id >= RTE_MAX_LCORE)
				rte_exit(EXIT_FAILURE, "Not enough cores\n");
		}

		if (qconf != &lcore_queue_conf[rx_lcore_id])
			/* Assigned a new logical core in the loop above. */
			qconf = &lcore_queue_conf[rx_lcore_id];


		rte_eth_macaddr_get(portid, &ctrl->vm_ports[port_nr].ethaddr);

		qconf->rx_port_list[qconf->n_rx_port] = portid;
		qconf->port_param[qconf->n_rx_port] = &ctrl->vm_ports[port_nr];
		qconf->n_rx_port++;
		port_nr++;
		printf("Lcore %u: RX port %u\n", rx_lcore_id, (unsigned) portid);
	}

	check_all_ports_link_status(nb_ports_available, l2fwd_ivshmem_enabled_port_mask);

	/* create rings for each VM port (several ports can be on the same VM).
	 * note that we store the pointers in ctrl - that way, they are the same
	 * and valid across all VMs because ctrl is also in DPDK memory */
	for (portid = 0; portid < nb_ports_available; portid++) {

		/* RX ring. SP/SC because it's only used by host and a single VM */
		snprintf(name, sizeof(name), "%s%i", RX_RING_PREFIX, portid);
		r = rte_ring_create(name, NB_MBUF,
				SOCKET_ID_ANY, RING_F_SP_ENQ | RING_F_SC_DEQ);
		if (r == NULL)
			rte_exit(EXIT_FAILURE, "Cannot create ring %s\n", name);

		ctrl->vm_ports[portid].rx_ring = r;

		/* TX ring. SP/SC because it's only used by host and a single VM */
		snprintf(name, sizeof(name), "%s%i", TX_RING_PREFIX, portid);
		r = rte_ring_create(name, NB_MBUF,
				SOCKET_ID_ANY, RING_F_SP_ENQ | RING_F_SC_DEQ);
		if (r == NULL)
			rte_exit(EXIT_FAILURE, "Cannot create ring %s\n", name);

		ctrl->vm_ports[portid].tx_ring = r;
	}

	/* create metadata, output cmdline */
	if (rte_ivshmem_metadata_create(METADATA_NAME) < 0)
		rte_exit(EXIT_FAILURE, "Cannot create IVSHMEM metadata\n");

	if (rte_ivshmem_metadata_add_memzone(ctrl_mz, METADATA_NAME))
		rte_exit(EXIT_FAILURE, "Cannot add memzone to IVSHMEM metadata\n");

	if (rte_ivshmem_metadata_add_mempool(l2fwd_ivshmem_pktmbuf_pool, METADATA_NAME))
		rte_exit(EXIT_FAILURE, "Cannot add mbuf mempool to IVSHMEM metadata\n");

	for (portid = 0; portid < nb_ports_available; portid++) {
		if (rte_ivshmem_metadata_add_ring(ctrl->vm_ports[portid].rx_ring,
				METADATA_NAME) < 0)
			rte_exit(EXIT_FAILURE, "Cannot add ring %s to IVSHMEM metadata\n",
					ctrl->vm_ports[portid].rx_ring->name);
		if (rte_ivshmem_metadata_add_ring(ctrl->vm_ports[portid].tx_ring,
				METADATA_NAME) < 0)
			rte_exit(EXIT_FAILURE, "Cannot add ring %s to IVSHMEM metadata\n",
					ctrl->vm_ports[portid].tx_ring->name);
	}
	generate_ivshmem_cmdline(METADATA_NAME);

	ctrl->nb_ports = nb_ports_available;

	printf("Waiting for VM to initialize...\n");

	/* wait for VM to initialize */
	while (ctrl->state != STATE_FWD) {
		if (ctrl->state == STATE_FAIL)
			rte_exit(EXIT_FAILURE, "VM reported failure\n");

		sleep(1);
	}

	printf("Done!\n");

	sigsetup();

	/* launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(l2fwd_ivshmem_launch_one_lcore, NULL, CALL_MASTER);
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			return -1;
	}

	if (ctrl->state == STATE_FAIL)
		rte_exit(EXIT_FAILURE, "VM reported failure\n");

	return 0;
}
