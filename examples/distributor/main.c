/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2017 Intel Corporation. All rights reserved.
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

#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_malloc.h>
#include <rte_debug.h>
#include <rte_prefetch.h>
#include <rte_distributor.h>
#include <rte_pause.h>

#define RX_RING_SIZE 512
#define TX_RING_SIZE 512
#define NUM_MBUFS ((64*1024)-1)
#define MBUF_CACHE_SIZE 128
#define BURST_SIZE 64
#define SCHED_RX_RING_SZ 8192
#define SCHED_TX_RING_SZ 65536
#define BURST_SIZE_TX 32

#define RTE_LOGTYPE_DISTRAPP RTE_LOGTYPE_USER1

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_RESET   "\x1b[0m"

/* mask of enabled ports */
static uint32_t enabled_port_mask;
volatile uint8_t quit_signal;
volatile uint8_t quit_signal_rx;
volatile uint8_t quit_signal_dist;
volatile uint8_t quit_signal_work;

static volatile struct app_stats {
	struct {
		uint64_t rx_pkts;
		uint64_t returned_pkts;
		uint64_t enqueued_pkts;
		uint64_t enqdrop_pkts;
	} rx __rte_cache_aligned;
	int pad1 __rte_cache_aligned;

	struct {
		uint64_t in_pkts;
		uint64_t ret_pkts;
		uint64_t sent_pkts;
		uint64_t enqdrop_pkts;
	} dist __rte_cache_aligned;
	int pad2 __rte_cache_aligned;

	struct {
		uint64_t dequeue_pkts;
		uint64_t tx_pkts;
		uint64_t enqdrop_pkts;
	} tx __rte_cache_aligned;
	int pad3 __rte_cache_aligned;

	uint64_t worker_pkts[64] __rte_cache_aligned;

	int pad4 __rte_cache_aligned;

	uint64_t worker_bursts[64][8] __rte_cache_aligned;

	int pad5 __rte_cache_aligned;

	uint64_t port_rx_pkts[64] __rte_cache_aligned;
	uint64_t port_tx_pkts[64] __rte_cache_aligned;
} app_stats;

struct app_stats prev_app_stats;

static const struct rte_eth_conf port_conf_default = {
	.rxmode = {
		.mq_mode = ETH_MQ_RX_RSS,
		.max_rx_pkt_len = ETHER_MAX_LEN,
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
	.rx_adv_conf = {
		.rss_conf = {
			.rss_hf = ETH_RSS_IP | ETH_RSS_UDP |
				ETH_RSS_TCP | ETH_RSS_SCTP,
		}
	},
};

struct output_buffer {
	unsigned count;
	struct rte_mbuf *mbufs[BURST_SIZE];
};

static void print_stats(void);

/*
 * Initialises a given port using global settings and with the rx buffers
 * coming from the mbuf_pool passed as parameter
 */
static inline int
port_init(uint8_t port, struct rte_mempool *mbuf_pool)
{
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rxRings = 1, txRings = rte_lcore_count() - 1;
	int retval;
	uint16_t q;
	uint16_t nb_rxd = RX_RING_SIZE;
	uint16_t nb_txd = TX_RING_SIZE;

	if (port >= rte_eth_dev_count())
		return -1;

	retval = rte_eth_dev_configure(port, rxRings, txRings, &port_conf);
	if (retval != 0)
		return retval;

	retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
	if (retval != 0)
		return retval;

	for (q = 0; q < rxRings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, nb_rxd,
						rte_eth_dev_socket_id(port),
						NULL, mbuf_pool);
		if (retval < 0)
			return retval;
	}

	for (q = 0; q < txRings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, nb_txd,
						rte_eth_dev_socket_id(port),
						NULL);
		if (retval < 0)
			return retval;
	}

	retval = rte_eth_dev_start(port);
	if (retval < 0)
		return retval;

	struct rte_eth_link link;
	rte_eth_link_get_nowait(port, &link);
	while (!link.link_status) {
		printf("Waiting for Link up on port %"PRIu8"\n", port);
		sleep(1);
		rte_eth_link_get_nowait(port, &link);
	}

	if (!link.link_status) {
		printf("Link down on port %"PRIu8"\n", port);
		return 0;
	}

	struct ether_addr addr;
	rte_eth_macaddr_get(port, &addr);
	printf("Port %u MAC: %02"PRIx8" %02"PRIx8" %02"PRIx8
			" %02"PRIx8" %02"PRIx8" %02"PRIx8"\n",
			(unsigned)port,
			addr.addr_bytes[0], addr.addr_bytes[1],
			addr.addr_bytes[2], addr.addr_bytes[3],
			addr.addr_bytes[4], addr.addr_bytes[5]);

	rte_eth_promiscuous_enable(port);

	return 0;
}

struct lcore_params {
	unsigned worker_id;
	struct rte_distributor *d;
	struct rte_ring *rx_dist_ring;
	struct rte_ring *dist_tx_ring;
	struct rte_mempool *mem_pool;
};

static int
lcore_rx(struct lcore_params *p)
{
	const uint8_t nb_ports = rte_eth_dev_count();
	const int socket_id = rte_socket_id();
	uint8_t port;
	struct rte_mbuf *bufs[BURST_SIZE*2];

	for (port = 0; port < nb_ports; port++) {
		/* skip ports that are not enabled */
		if ((enabled_port_mask & (1 << port)) == 0)
			continue;

		if (rte_eth_dev_socket_id(port) > 0 &&
				rte_eth_dev_socket_id(port) != socket_id)
			printf("WARNING, port %u is on remote NUMA node to "
					"RX thread.\n\tPerformance will not "
					"be optimal.\n", port);
	}

	printf("\nCore %u doing packet RX.\n", rte_lcore_id());
	port = 0;
	while (!quit_signal_rx) {

		/* skip ports that are not enabled */
		if ((enabled_port_mask & (1 << port)) == 0) {
			if (++port == nb_ports)
				port = 0;
			continue;
		}
		const uint16_t nb_rx = rte_eth_rx_burst(port, 0, bufs,
				BURST_SIZE);
		if (unlikely(nb_rx == 0)) {
			if (++port == nb_ports)
				port = 0;
			continue;
		}
		app_stats.rx.rx_pkts += nb_rx;

/*
 * You can run the distributor on the rx core with this code. Returned
 * packets are then send straight to the tx core.
 */
#if 0
	rte_distributor_process(d, bufs, nb_rx);
	const uint16_t nb_ret = rte_distributor_returned_pktsd,
			bufs, BURST_SIZE*2);

		app_stats.rx.returned_pkts += nb_ret;
		if (unlikely(nb_ret == 0)) {
			if (++port == nb_ports)
				port = 0;
			continue;
		}

		struct rte_ring *tx_ring = p->dist_tx_ring;
		uint16_t sent = rte_ring_enqueue_burst(tx_ring,
				(void *)bufs, nb_ret, NULL);
#else
		uint16_t nb_ret = nb_rx;
		/*
		 * Swap the following two lines if you want the rx traffic
		 * to go directly to tx, no distribution.
		 */
		struct rte_ring *out_ring = p->rx_dist_ring;
		/* struct rte_ring *out_ring = p->dist_tx_ring; */

		uint16_t sent = rte_ring_enqueue_burst(out_ring,
				(void *)bufs, nb_ret, NULL);
#endif

		app_stats.rx.enqueued_pkts += sent;
		if (unlikely(sent < nb_ret)) {
			app_stats.rx.enqdrop_pkts +=  nb_ret - sent;
			RTE_LOG_DP(DEBUG, DISTRAPP,
				"%s:Packet loss due to full ring\n", __func__);
			while (sent < nb_ret)
				rte_pktmbuf_free(bufs[sent++]);
		}
		if (++port == nb_ports)
			port = 0;
	}
	/* set worker & tx threads quit flag */
	printf("\nCore %u exiting rx task.\n", rte_lcore_id());
	quit_signal = 1;
	return 0;
}

static inline void
flush_one_port(struct output_buffer *outbuf, uint8_t outp)
{
	unsigned int nb_tx = rte_eth_tx_burst(outp, 0,
			outbuf->mbufs, outbuf->count);
	app_stats.tx.tx_pkts += outbuf->count;

	if (unlikely(nb_tx < outbuf->count)) {
		app_stats.tx.enqdrop_pkts +=  outbuf->count - nb_tx;
		do {
			rte_pktmbuf_free(outbuf->mbufs[nb_tx]);
		} while (++nb_tx < outbuf->count);
	}
	outbuf->count = 0;
}

static inline void
flush_all_ports(struct output_buffer *tx_buffers, uint8_t nb_ports)
{
	uint8_t outp;

	for (outp = 0; outp < nb_ports; outp++) {
		/* skip ports that are not enabled */
		if ((enabled_port_mask & (1 << outp)) == 0)
			continue;

		if (tx_buffers[outp].count == 0)
			continue;

		flush_one_port(&tx_buffers[outp], outp);
	}
}



static int
lcore_distributor(struct lcore_params *p)
{
	struct rte_ring *in_r = p->rx_dist_ring;
	struct rte_ring *out_r = p->dist_tx_ring;
	struct rte_mbuf *bufs[BURST_SIZE * 4];
	struct rte_distributor *d = p->d;

	printf("\nCore %u acting as distributor core.\n", rte_lcore_id());
	while (!quit_signal_dist) {
		const uint16_t nb_rx = rte_ring_dequeue_burst(in_r,
				(void *)bufs, BURST_SIZE*1, NULL);
		if (nb_rx) {
			app_stats.dist.in_pkts += nb_rx;

			/* Distribute the packets */
			rte_distributor_process(d, bufs, nb_rx);
			/* Handle Returns */
			const uint16_t nb_ret =
				rte_distributor_returned_pkts(d,
					bufs, BURST_SIZE*2);

			if (unlikely(nb_ret == 0))
				continue;
			app_stats.dist.ret_pkts += nb_ret;

			uint16_t sent = rte_ring_enqueue_burst(out_r,
					(void *)bufs, nb_ret, NULL);
			app_stats.dist.sent_pkts += sent;
			if (unlikely(sent < nb_ret)) {
				app_stats.dist.enqdrop_pkts += nb_ret - sent;
				RTE_LOG(DEBUG, DISTRAPP,
					"%s:Packet loss due to full out ring\n",
					__func__);
				while (sent < nb_ret)
					rte_pktmbuf_free(bufs[sent++]);
			}
		}
	}
	printf("\nCore %u exiting distributor task.\n", rte_lcore_id());
	quit_signal_work = 1;

	rte_distributor_flush(d);
	/* Unblock any returns so workers can exit */
	rte_distributor_clear_returns(d);
	quit_signal_rx = 1;
	return 0;
}


static int
lcore_tx(struct rte_ring *in_r)
{
	static struct output_buffer tx_buffers[RTE_MAX_ETHPORTS];
	const uint8_t nb_ports = rte_eth_dev_count();
	const int socket_id = rte_socket_id();
	uint8_t port;

	for (port = 0; port < nb_ports; port++) {
		/* skip ports that are not enabled */
		if ((enabled_port_mask & (1 << port)) == 0)
			continue;

		if (rte_eth_dev_socket_id(port) > 0 &&
				rte_eth_dev_socket_id(port) != socket_id)
			printf("WARNING, port %u is on remote NUMA node to "
					"TX thread.\n\tPerformance will not "
					"be optimal.\n", port);
	}

	printf("\nCore %u doing packet TX.\n", rte_lcore_id());
	while (!quit_signal) {

		for (port = 0; port < nb_ports; port++) {
			/* skip ports that are not enabled */
			if ((enabled_port_mask & (1 << port)) == 0)
				continue;

			struct rte_mbuf *bufs[BURST_SIZE_TX];
			const uint16_t nb_rx = rte_ring_dequeue_burst(in_r,
					(void *)bufs, BURST_SIZE_TX, NULL);
			app_stats.tx.dequeue_pkts += nb_rx;

			/* if we get no traffic, flush anything we have */
			if (unlikely(nb_rx == 0)) {
				flush_all_ports(tx_buffers, nb_ports);
				continue;
			}

			/* for traffic we receive, queue it up for transmit */
			uint16_t i;
			rte_prefetch_non_temporal((void *)bufs[0]);
			rte_prefetch_non_temporal((void *)bufs[1]);
			rte_prefetch_non_temporal((void *)bufs[2]);
			for (i = 0; i < nb_rx; i++) {
				struct output_buffer *outbuf;
				uint8_t outp;
				rte_prefetch_non_temporal((void *)bufs[i + 3]);
				/*
				 * workers should update in_port to hold the
				 * output port value
				 */
				outp = bufs[i]->port;
				/* skip ports that are not enabled */
				if ((enabled_port_mask & (1 << outp)) == 0)
					continue;

				outbuf = &tx_buffers[outp];
				outbuf->mbufs[outbuf->count++] = bufs[i];
				if (outbuf->count == BURST_SIZE_TX)
					flush_one_port(outbuf, outp);
			}
		}
	}
	printf("\nCore %u exiting tx task.\n", rte_lcore_id());
	return 0;
}

static void
int_handler(int sig_num)
{
	printf("Exiting on signal %d\n", sig_num);
	/* set quit flag for rx thread to exit */
	quit_signal_dist = 1;
}

static void
print_stats(void)
{
	struct rte_eth_stats eth_stats;
	unsigned int i, j;
	const unsigned int num_workers = rte_lcore_count() - 4;

	for (i = 0; i < rte_eth_dev_count(); i++) {
		rte_eth_stats_get(i, &eth_stats);
		app_stats.port_rx_pkts[i] = eth_stats.ipackets;
		app_stats.port_tx_pkts[i] = eth_stats.opackets;
	}

	printf("\n\nRX Thread:\n");
	for (i = 0; i < rte_eth_dev_count(); i++) {
		printf("Port %u Pktsin : %5.2f\n", i,
				(app_stats.port_rx_pkts[i] -
				prev_app_stats.port_rx_pkts[i])/1000000.0);
		prev_app_stats.port_rx_pkts[i] = app_stats.port_rx_pkts[i];
	}
	printf(" - Received:    %5.2f\n",
			(app_stats.rx.rx_pkts -
			prev_app_stats.rx.rx_pkts)/1000000.0);
	printf(" - Returned:    %5.2f\n",
			(app_stats.rx.returned_pkts -
			prev_app_stats.rx.returned_pkts)/1000000.0);
	printf(" - Enqueued:    %5.2f\n",
			(app_stats.rx.enqueued_pkts -
			prev_app_stats.rx.enqueued_pkts)/1000000.0);
	printf(" - Dropped:     %s%5.2f%s\n", ANSI_COLOR_RED,
			(app_stats.rx.enqdrop_pkts -
			prev_app_stats.rx.enqdrop_pkts)/1000000.0,
			ANSI_COLOR_RESET);

	printf("Distributor thread:\n");
	printf(" - In:          %5.2f\n",
			(app_stats.dist.in_pkts -
			prev_app_stats.dist.in_pkts)/1000000.0);
	printf(" - Returned:    %5.2f\n",
			(app_stats.dist.ret_pkts -
			prev_app_stats.dist.ret_pkts)/1000000.0);
	printf(" - Sent:        %5.2f\n",
			(app_stats.dist.sent_pkts -
			prev_app_stats.dist.sent_pkts)/1000000.0);
	printf(" - Dropped      %s%5.2f%s\n", ANSI_COLOR_RED,
			(app_stats.dist.enqdrop_pkts -
			prev_app_stats.dist.enqdrop_pkts)/1000000.0,
			ANSI_COLOR_RESET);

	printf("TX thread:\n");
	printf(" - Dequeued:    %5.2f\n",
			(app_stats.tx.dequeue_pkts -
			prev_app_stats.tx.dequeue_pkts)/1000000.0);
	for (i = 0; i < rte_eth_dev_count(); i++) {
		printf("Port %u Pktsout: %5.2f\n",
				i, (app_stats.port_tx_pkts[i] -
				prev_app_stats.port_tx_pkts[i])/1000000.0);
		prev_app_stats.port_tx_pkts[i] = app_stats.port_tx_pkts[i];
	}
	printf(" - Transmitted: %5.2f\n",
			(app_stats.tx.tx_pkts -
			prev_app_stats.tx.tx_pkts)/1000000.0);
	printf(" - Dropped:     %s%5.2f%s\n", ANSI_COLOR_RED,
			(app_stats.tx.enqdrop_pkts -
			prev_app_stats.tx.enqdrop_pkts)/1000000.0,
			ANSI_COLOR_RESET);

	prev_app_stats.rx.rx_pkts = app_stats.rx.rx_pkts;
	prev_app_stats.rx.returned_pkts = app_stats.rx.returned_pkts;
	prev_app_stats.rx.enqueued_pkts = app_stats.rx.enqueued_pkts;
	prev_app_stats.rx.enqdrop_pkts = app_stats.rx.enqdrop_pkts;
	prev_app_stats.dist.in_pkts = app_stats.dist.in_pkts;
	prev_app_stats.dist.ret_pkts = app_stats.dist.ret_pkts;
	prev_app_stats.dist.sent_pkts = app_stats.dist.sent_pkts;
	prev_app_stats.dist.enqdrop_pkts = app_stats.dist.enqdrop_pkts;
	prev_app_stats.tx.dequeue_pkts = app_stats.tx.dequeue_pkts;
	prev_app_stats.tx.tx_pkts = app_stats.tx.tx_pkts;
	prev_app_stats.tx.enqdrop_pkts = app_stats.tx.enqdrop_pkts;

	for (i = 0; i < num_workers; i++) {
		printf("Worker %02u Pkts: %5.2f. Bursts(1-8): ", i,
				(app_stats.worker_pkts[i] -
				prev_app_stats.worker_pkts[i])/1000000.0);
		for (j = 0; j < 8; j++) {
			printf("%"PRIu64" ", app_stats.worker_bursts[i][j]);
			app_stats.worker_bursts[i][j] = 0;
		}
		printf("\n");
		prev_app_stats.worker_pkts[i] = app_stats.worker_pkts[i];
	}
}

static int
lcore_worker(struct lcore_params *p)
{
	struct rte_distributor *d = p->d;
	const unsigned id = p->worker_id;
	unsigned int num = 0;
	unsigned int i;

	/*
	 * for single port, xor_val will be zero so we won't modify the output
	 * port, otherwise we send traffic from 0 to 1, 2 to 3, and vice versa
	 */
	const unsigned xor_val = (rte_eth_dev_count() > 1);
	struct rte_mbuf *buf[8] __rte_cache_aligned;

	for (i = 0; i < 8; i++)
		buf[i] = NULL;

	app_stats.worker_pkts[p->worker_id] = 1;

	printf("\nCore %u acting as worker core.\n", rte_lcore_id());
	while (!quit_signal_work) {
		num = rte_distributor_get_pkt(d, id, buf, buf, num);
		/* Do a little bit of work for each packet */
		for (i = 0; i < num; i++) {
			uint64_t t = rte_rdtsc()+100;

			while (rte_rdtsc() < t)
				rte_pause();
			buf[i]->port ^= xor_val;
		}

		app_stats.worker_pkts[p->worker_id] += num;
		if (num > 0)
			app_stats.worker_bursts[p->worker_id][num-1]++;
	}
	return 0;
}

/* display usage */
static void
print_usage(const char *prgname)
{
	printf("%s [EAL options] -- -p PORTMASK\n"
			"  -p PORTMASK: hexadecimal bitmask of ports to configure\n",
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

/* Parse the argument given in the command line of the application */
static int
parse_args(int argc, char **argv)
{
	int opt;
	char **argvopt;
	int option_index;
	char *prgname = argv[0];
	static struct option lgopts[] = {
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

		default:
			print_usage(prgname);
			return -1;
		}
	}

	if (optind <= 1) {
		print_usage(prgname);
		return -1;
	}

	argv[optind-1] = prgname;

	optind = 1; /* reset getopt lib */
	return 0;
}

/* Main function, does initialization and calls the per-lcore functions */
int
main(int argc, char *argv[])
{
	struct rte_mempool *mbuf_pool;
	struct rte_distributor *d;
	struct rte_ring *dist_tx_ring;
	struct rte_ring *rx_dist_ring;
	unsigned lcore_id, worker_id = 0;
	unsigned nb_ports;
	uint8_t portid;
	uint8_t nb_ports_available;
	uint64_t t, freq;

	/* catch ctrl-c so we can print on exit */
	signal(SIGINT, int_handler);

	/* init EAL */
	int ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
	argc -= ret;
	argv += ret;

	/* parse application arguments (after the EAL ones) */
	ret = parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid distributor parameters\n");

	if (rte_lcore_count() < 5)
		rte_exit(EXIT_FAILURE, "Error, This application needs at "
				"least 5 logical cores to run:\n"
				"1 lcore for stats (can be core 0)\n"
				"1 lcore for packet RX\n"
				"1 lcore for distribution\n"
				"1 lcore for packet TX\n"
				"and at least 1 lcore for worker threads\n");

	nb_ports = rte_eth_dev_count();
	if (nb_ports == 0)
		rte_exit(EXIT_FAILURE, "Error: no ethernet ports detected\n");
	if (nb_ports != 1 && (nb_ports & 1))
		rte_exit(EXIT_FAILURE, "Error: number of ports must be even, except "
				"when using a single port\n");

	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL",
		NUM_MBUFS * nb_ports, MBUF_CACHE_SIZE, 0,
		RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");
	nb_ports_available = nb_ports;

	/* initialize all ports */
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if ((enabled_port_mask & (1 << portid)) == 0) {
			printf("\nSkipping disabled port %d\n", portid);
			nb_ports_available--;
			continue;
		}
		/* init port */
		printf("Initializing port %u... done\n", (unsigned) portid);

		if (port_init(portid, mbuf_pool) != 0)
			rte_exit(EXIT_FAILURE, "Cannot initialize port %"PRIu8"\n",
					portid);
	}

	if (!nb_ports_available) {
		rte_exit(EXIT_FAILURE,
				"All available ports are disabled. Please set portmask.\n");
	}

	d = rte_distributor_create("PKT_DIST", rte_socket_id(),
			rte_lcore_count() - 4,
			RTE_DIST_ALG_BURST);
	if (d == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create distributor\n");

	/*
	 * scheduler ring is read by the transmitter core, and written to
	 * by scheduler core
	 */
	dist_tx_ring = rte_ring_create("Output_ring", SCHED_TX_RING_SZ,
			rte_socket_id(), RING_F_SC_DEQ | RING_F_SP_ENQ);
	if (dist_tx_ring == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create output ring\n");

	rx_dist_ring = rte_ring_create("Input_ring", SCHED_RX_RING_SZ,
			rte_socket_id(), RING_F_SC_DEQ | RING_F_SP_ENQ);
	if (rx_dist_ring == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create output ring\n");

	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (worker_id == rte_lcore_count() - 3) {
			printf("Starting distributor on lcore_id %d\n",
					lcore_id);
			/* distributor core */
			struct lcore_params *p =
					rte_malloc(NULL, sizeof(*p), 0);
			if (!p)
				rte_panic("malloc failure\n");
			*p = (struct lcore_params){worker_id, d,
				rx_dist_ring, dist_tx_ring, mbuf_pool};
			rte_eal_remote_launch(
				(lcore_function_t *)lcore_distributor,
				p, lcore_id);
		} else if (worker_id == rte_lcore_count() - 4) {
			printf("Starting tx  on worker_id %d, lcore_id %d\n",
					worker_id, lcore_id);
			/* tx core */
			rte_eal_remote_launch((lcore_function_t *)lcore_tx,
					dist_tx_ring, lcore_id);
		} else if (worker_id == rte_lcore_count() - 2) {
			printf("Starting rx on worker_id %d, lcore_id %d\n",
					worker_id, lcore_id);
			/* rx core */
			struct lcore_params *p =
					rte_malloc(NULL, sizeof(*p), 0);
			if (!p)
				rte_panic("malloc failure\n");
			*p = (struct lcore_params){worker_id, d, rx_dist_ring,
					dist_tx_ring, mbuf_pool};
			rte_eal_remote_launch((lcore_function_t *)lcore_rx,
					p, lcore_id);
		} else {
			printf("Starting worker on worker_id %d, lcore_id %d\n",
					worker_id, lcore_id);
			struct lcore_params *p =
					rte_malloc(NULL, sizeof(*p), 0);
			if (!p)
				rte_panic("malloc failure\n");
			*p = (struct lcore_params){worker_id, d, rx_dist_ring,
					dist_tx_ring, mbuf_pool};

			rte_eal_remote_launch((lcore_function_t *)lcore_worker,
					p, lcore_id);
		}
		worker_id++;
	}

	freq = rte_get_timer_hz();
	t = rte_rdtsc() + freq;
	while (!quit_signal_dist) {
		if (t < rte_rdtsc()) {
			print_stats();
			t = rte_rdtsc() + freq;
		}
		usleep(1000);
	}

	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			return -1;
	}

	print_stats();
	return 0;
}
