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
#include <rte_distributor.h>

#define RX_RING_SIZE 256
#define TX_RING_SIZE 512
#define NUM_MBUFS ((64*1024)-1)
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32
#define RTE_RING_SZ 1024

/* uncommnet below line to enable debug logs */
/* #define DEBUG */

#ifdef DEBUG
#define LOG_LEVEL RTE_LOG_DEBUG
#define LOG_DEBUG(log_type, fmt, args...) do {	\
	RTE_LOG(DEBUG, log_type, fmt, ##args);		\
} while (0)
#else
#define LOG_LEVEL RTE_LOG_INFO
#define LOG_DEBUG(log_type, fmt, args...) do {} while (0)
#endif

#define RTE_LOGTYPE_DISTRAPP RTE_LOGTYPE_USER1

/* mask of enabled ports */
static uint32_t enabled_port_mask;
volatile uint8_t quit_signal;
volatile uint8_t quit_signal_rx;

static volatile struct app_stats {
	struct {
		uint64_t rx_pkts;
		uint64_t returned_pkts;
		uint64_t enqueued_pkts;
	} rx __rte_cache_aligned;

	struct {
		uint64_t dequeue_pkts;
		uint64_t tx_pkts;
	} tx __rte_cache_aligned;
} app_stats;

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

	if (port >= rte_eth_dev_count())
		return -1;

	retval = rte_eth_dev_configure(port, rxRings, txRings, &port_conf);
	if (retval != 0)
		return retval;

	for (q = 0; q < rxRings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, RX_RING_SIZE,
						rte_eth_dev_socket_id(port),
						NULL, mbuf_pool);
		if (retval < 0)
			return retval;
	}

	for (q = 0; q < txRings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, TX_RING_SIZE,
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
	if (!link.link_status) {
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
	struct rte_ring *r;
	struct rte_mempool *mem_pool;
};

static void
quit_workers(struct rte_distributor *d, struct rte_mempool *p)
{
	const unsigned num_workers = rte_lcore_count() - 2;
	unsigned i;
	struct rte_mbuf *bufs[num_workers];
	rte_mempool_get_bulk(p, (void *)bufs, num_workers);

	for (i = 0; i < num_workers; i++)
		bufs[i]->hash.rss = i << 1;

	rte_distributor_process(d, bufs, num_workers);
	rte_mempool_put_bulk(p, (void *)bufs, num_workers);
}

static int
lcore_rx(struct lcore_params *p)
{
	struct rte_distributor *d = p->d;
	struct rte_mempool *mem_pool = p->mem_pool;
	struct rte_ring *r = p->r;
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
		struct rte_mbuf *bufs[BURST_SIZE*2];
		const uint16_t nb_rx = rte_eth_rx_burst(port, 0, bufs,
				BURST_SIZE);
		app_stats.rx.rx_pkts += nb_rx;

		rte_distributor_process(d, bufs, nb_rx);
		const uint16_t nb_ret = rte_distributor_returned_pkts(d,
				bufs, BURST_SIZE*2);
		app_stats.rx.returned_pkts += nb_ret;
		if (unlikely(nb_ret == 0))
			continue;

		uint16_t sent = rte_ring_enqueue_burst(r, (void *)bufs, nb_ret);
		app_stats.rx.enqueued_pkts += sent;
		if (unlikely(sent < nb_ret)) {
			LOG_DEBUG(DISTRAPP, "%s:Packet loss due to full ring\n", __func__);
			while (sent < nb_ret)
				rte_pktmbuf_free(bufs[sent++]);
		}
		if (++port == nb_ports)
			port = 0;
	}
	rte_distributor_process(d, NULL, 0);
	/* flush distributor to bring to known state */
	rte_distributor_flush(d);
	/* set worker & tx threads quit flag */
	quit_signal = 1;
	/*
	 * worker threads may hang in get packet as
	 * distributor process is not running, just make sure workers
	 * get packets till quit_signal is actually been
	 * received and they gracefully shutdown
	 */
	quit_workers(d, mem_pool);
	/* rx thread should quit at last */
	return 0;
}

static inline void
flush_one_port(struct output_buffer *outbuf, uint8_t outp)
{
	unsigned nb_tx = rte_eth_tx_burst(outp, 0, outbuf->mbufs,
			outbuf->count);
	app_stats.tx.tx_pkts += nb_tx;

	if (unlikely(nb_tx < outbuf->count)) {
		LOG_DEBUG(DISTRAPP, "%s:Packet loss with tx_burst\n", __func__);
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

			struct rte_mbuf *bufs[BURST_SIZE];
			const uint16_t nb_rx = rte_ring_dequeue_burst(in_r,
					(void *)bufs, BURST_SIZE);
			app_stats.tx.dequeue_pkts += nb_rx;

			/* if we get no traffic, flush anything we have */
			if (unlikely(nb_rx == 0)) {
				flush_all_ports(tx_buffers, nb_ports);
				continue;
			}

			/* for traffic we receive, queue it up for transmit */
			uint16_t i;
			_mm_prefetch(bufs[0], 0);
			_mm_prefetch(bufs[1], 0);
			_mm_prefetch(bufs[2], 0);
			for (i = 0; i < nb_rx; i++) {
				struct output_buffer *outbuf;
				uint8_t outp;
				_mm_prefetch(bufs[i + 3], 0);
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
				if (outbuf->count == BURST_SIZE)
					flush_one_port(outbuf, outp);
			}
		}
	}
	return 0;
}

static void
int_handler(int sig_num)
{
	printf("Exiting on signal %d\n", sig_num);
	/* set quit flag for rx thread to exit */
	quit_signal_rx = 1;
}

static void
print_stats(void)
{
	struct rte_eth_stats eth_stats;
	unsigned i;

	printf("\nRX thread stats:\n");
	printf(" - Received:    %"PRIu64"\n", app_stats.rx.rx_pkts);
	printf(" - Processed:   %"PRIu64"\n", app_stats.rx.returned_pkts);
	printf(" - Enqueued:    %"PRIu64"\n", app_stats.rx.enqueued_pkts);

	printf("\nTX thread stats:\n");
	printf(" - Dequeued:    %"PRIu64"\n", app_stats.tx.dequeue_pkts);
	printf(" - Transmitted: %"PRIu64"\n", app_stats.tx.tx_pkts);

	for (i = 0; i < rte_eth_dev_count(); i++) {
		rte_eth_stats_get(i, &eth_stats);
		printf("\nPort %u stats:\n", i);
		printf(" - Pkts in:   %"PRIu64"\n", eth_stats.ipackets);
		printf(" - Pkts out:  %"PRIu64"\n", eth_stats.opackets);
		printf(" - In Errs:   %"PRIu64"\n", eth_stats.ierrors);
		printf(" - Out Errs:  %"PRIu64"\n", eth_stats.oerrors);
		printf(" - Mbuf Errs: %"PRIu64"\n", eth_stats.rx_nombuf);
	}
}

static int
lcore_worker(struct lcore_params *p)
{
	struct rte_distributor *d = p->d;
	const unsigned id = p->worker_id;
	/*
	 * for single port, xor_val will be zero so we won't modify the output
	 * port, otherwise we send traffic from 0 to 1, 2 to 3, and vice versa
	 */
	const unsigned xor_val = (rte_eth_dev_count() > 1);
	struct rte_mbuf *buf = NULL;

	printf("\nCore %u acting as worker core.\n", rte_lcore_id());
	while (!quit_signal) {
		buf = rte_distributor_get_pkt(d, id, buf);
		buf->port ^= xor_val;
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

	optind = 0; /* reset getopt lib */
	return 0;
}

/* Main function, does initialization and calls the per-lcore functions */
int
main(int argc, char *argv[])
{
	struct rte_mempool *mbuf_pool;
	struct rte_distributor *d;
	struct rte_ring *output_ring;
	unsigned lcore_id, worker_id = 0;
	unsigned nb_ports;
	uint8_t portid;
	uint8_t nb_ports_available;

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

	if (rte_lcore_count() < 3)
		rte_exit(EXIT_FAILURE, "Error, This application needs at "
				"least 3 logical cores to run:\n"
				"1 lcore for packet RX and distribution\n"
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
			rte_lcore_count() - 2);
	if (d == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create distributor\n");

	/*
	 * scheduler ring is read only by the transmitter core, but written to
	 * by multiple threads
	 */
	output_ring = rte_ring_create("Output_ring", RTE_RING_SZ,
			rte_socket_id(), RING_F_SC_DEQ);
	if (output_ring == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create output ring\n");

	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (worker_id == rte_lcore_count() - 2)
			rte_eal_remote_launch((lcore_function_t *)lcore_tx,
					output_ring, lcore_id);
		else {
			struct lcore_params *p =
					rte_malloc(NULL, sizeof(*p), 0);
			if (!p)
				rte_panic("malloc failure\n");
			*p = (struct lcore_params){worker_id, d, output_ring, mbuf_pool};

			rte_eal_remote_launch((lcore_function_t *)lcore_worker,
					p, lcore_id);
		}
		worker_id++;
	}
	/* call lcore_main on master core only */
	struct lcore_params p = { 0, d, output_ring, mbuf_pool};
	lcore_rx(&p);

	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			return -1;
	}

	print_stats();
	return 0;
}
