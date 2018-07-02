/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#include <signal.h>
#include <getopt.h>

#include <rte_eal.h>
#include <rte_common.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_ring.h>
#include <rte_reorder.h>

#define RX_DESC_PER_QUEUE 1024
#define TX_DESC_PER_QUEUE 1024

#define MAX_PKTS_BURST 32
#define REORDER_BUFFER_SIZE 8192
#define MBUF_PER_POOL 65535
#define MBUF_POOL_CACHE_SIZE 250

#define RING_SIZE 16384

/* Macros for printing using RTE_LOG */
#define RTE_LOGTYPE_REORDERAPP          RTE_LOGTYPE_USER1

unsigned int portmask;
unsigned int disable_reorder;
volatile uint8_t quit_signal;

static struct rte_mempool *mbuf_pool;

static struct rte_eth_conf port_conf_default;

struct worker_thread_args {
	struct rte_ring *ring_in;
	struct rte_ring *ring_out;
};

struct send_thread_args {
	struct rte_ring *ring_in;
	struct rte_reorder_buffer *buffer;
};

volatile struct app_stats {
	struct {
		uint64_t rx_pkts;
		uint64_t enqueue_pkts;
		uint64_t enqueue_failed_pkts;
	} rx __rte_cache_aligned;

	struct {
		uint64_t dequeue_pkts;
		uint64_t enqueue_pkts;
		uint64_t enqueue_failed_pkts;
	} wkr __rte_cache_aligned;

	struct {
		uint64_t dequeue_pkts;
		/* Too early pkts transmitted directly w/o reordering */
		uint64_t early_pkts_txtd_woro;
		/* Too early pkts failed from direct transmit */
		uint64_t early_pkts_tx_failed_woro;
		uint64_t ro_tx_pkts;
		uint64_t ro_tx_failed_pkts;
	} tx __rte_cache_aligned;
} app_stats;

/**
 * Get the last enabled lcore ID
 *
 * @return
 *   The last enabled lcore ID.
 */
static unsigned int
get_last_lcore_id(void)
{
	int i;

	for (i = RTE_MAX_LCORE - 1; i >= 0; i--)
		if (rte_lcore_is_enabled(i))
			return i;
	return 0;
}

/**
 * Get the previous enabled lcore ID
 * @param id
 *  The current lcore ID
 * @return
 *   The previous enabled lcore ID or the current lcore
 *   ID if it is the first available core.
 */
static unsigned int
get_previous_lcore_id(unsigned int id)
{
	int i;

	for (i = id - 1; i >= 0; i--)
		if (rte_lcore_is_enabled(i))
			return i;
	return id;
}

static inline void
pktmbuf_free_bulk(struct rte_mbuf *mbuf_table[], unsigned n)
{
	unsigned int i;

	for (i = 0; i < n; i++)
		rte_pktmbuf_free(mbuf_table[i]);
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
	unsigned long pm;
	char *end = NULL;

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
	int option_index;
	char **argvopt;
	char *prgname = argv[0];
	static struct option lgopts[] = {
		{"disable-reorder", 0, 0, 0},
		{NULL, 0, 0, 0}
	};

	argvopt = argv;

	while ((opt = getopt_long(argc, argvopt, "p:",
					lgopts, &option_index)) != EOF) {
		switch (opt) {
		/* portmask */
		case 'p':
			portmask = parse_portmask(optarg);
			if (portmask == 0) {
				printf("invalid portmask\n");
				print_usage(prgname);
				return -1;
			}
			break;
		/* long options */
		case 0:
			if (!strcmp(lgopts[option_index].name, "disable-reorder")) {
				printf("reorder disabled\n");
				disable_reorder = 1;
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

/*
 * Tx buffer error callback
 */
static void
flush_tx_error_callback(struct rte_mbuf **unsent, uint16_t count,
		void *userdata __rte_unused) {

	/* free the mbufs which failed from transmit */
	app_stats.tx.ro_tx_failed_pkts += count;
	RTE_LOG_DP(DEBUG, REORDERAPP, "%s:Packet loss with tx_burst\n", __func__);
	pktmbuf_free_bulk(unsent, count);

}

static inline int
free_tx_buffers(struct rte_eth_dev_tx_buffer *tx_buffer[]) {
	uint16_t port_id;

	/* initialize buffers for all ports */
	RTE_ETH_FOREACH_DEV(port_id) {
		/* skip ports that are not enabled */
		if ((portmask & (1 << port_id)) == 0)
			continue;

		rte_free(tx_buffer[port_id]);
	}
	return 0;
}

static inline int
configure_tx_buffers(struct rte_eth_dev_tx_buffer *tx_buffer[])
{
	uint16_t port_id;
	int ret;

	/* initialize buffers for all ports */
	RTE_ETH_FOREACH_DEV(port_id) {
		/* skip ports that are not enabled */
		if ((portmask & (1 << port_id)) == 0)
			continue;

		/* Initialize TX buffers */
		tx_buffer[port_id] = rte_zmalloc_socket("tx_buffer",
				RTE_ETH_TX_BUFFER_SIZE(MAX_PKTS_BURST), 0,
				rte_eth_dev_socket_id(port_id));
		if (tx_buffer[port_id] == NULL)
			rte_exit(EXIT_FAILURE, "Cannot allocate buffer for tx on port %u\n",
				 port_id);

		rte_eth_tx_buffer_init(tx_buffer[port_id], MAX_PKTS_BURST);

		ret = rte_eth_tx_buffer_set_err_callback(tx_buffer[port_id],
				flush_tx_error_callback, NULL);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
			"Cannot set error callback for tx buffer on port %u\n",
				 port_id);
	}
	return 0;
}

static inline int
configure_eth_port(uint16_t port_id)
{
	struct ether_addr addr;
	const uint16_t rxRings = 1, txRings = 1;
	int ret;
	uint16_t q;
	uint16_t nb_rxd = RX_DESC_PER_QUEUE;
	uint16_t nb_txd = TX_DESC_PER_QUEUE;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf txconf;
	struct rte_eth_conf port_conf = port_conf_default;

	if (!rte_eth_dev_is_valid_port(port_id))
		return -1;

	rte_eth_dev_info_get(port_id, &dev_info);
	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
		port_conf.txmode.offloads |=
			DEV_TX_OFFLOAD_MBUF_FAST_FREE;
	ret = rte_eth_dev_configure(port_id, rxRings, txRings, &port_conf_default);
	if (ret != 0)
		return ret;

	ret = rte_eth_dev_adjust_nb_rx_tx_desc(port_id, &nb_rxd, &nb_txd);
	if (ret != 0)
		return ret;

	for (q = 0; q < rxRings; q++) {
		ret = rte_eth_rx_queue_setup(port_id, q, nb_rxd,
				rte_eth_dev_socket_id(port_id), NULL,
				mbuf_pool);
		if (ret < 0)
			return ret;
	}

	txconf = dev_info.default_txconf;
	txconf.offloads = port_conf.txmode.offloads;
	for (q = 0; q < txRings; q++) {
		ret = rte_eth_tx_queue_setup(port_id, q, nb_txd,
				rte_eth_dev_socket_id(port_id), &txconf);
		if (ret < 0)
			return ret;
	}

	ret = rte_eth_dev_start(port_id);
	if (ret < 0)
		return ret;

	rte_eth_macaddr_get(port_id, &addr);
	printf("Port %u MAC: %02"PRIx8" %02"PRIx8" %02"PRIx8
			" %02"PRIx8" %02"PRIx8" %02"PRIx8"\n",
			port_id,
			addr.addr_bytes[0], addr.addr_bytes[1],
			addr.addr_bytes[2], addr.addr_bytes[3],
			addr.addr_bytes[4], addr.addr_bytes[5]);

	rte_eth_promiscuous_enable(port_id);

	return 0;
}

static void
print_stats(void)
{
	uint16_t i;
	struct rte_eth_stats eth_stats;

	printf("\nRX thread stats:\n");
	printf(" - Pkts rxd:				%"PRIu64"\n",
						app_stats.rx.rx_pkts);
	printf(" - Pkts enqd to workers ring:		%"PRIu64"\n",
						app_stats.rx.enqueue_pkts);

	printf("\nWorker thread stats:\n");
	printf(" - Pkts deqd from workers ring:		%"PRIu64"\n",
						app_stats.wkr.dequeue_pkts);
	printf(" - Pkts enqd to tx ring:		%"PRIu64"\n",
						app_stats.wkr.enqueue_pkts);
	printf(" - Pkts enq to tx failed:		%"PRIu64"\n",
						app_stats.wkr.enqueue_failed_pkts);

	printf("\nTX stats:\n");
	printf(" - Pkts deqd from tx ring:		%"PRIu64"\n",
						app_stats.tx.dequeue_pkts);
	printf(" - Ro Pkts transmitted:			%"PRIu64"\n",
						app_stats.tx.ro_tx_pkts);
	printf(" - Ro Pkts tx failed:			%"PRIu64"\n",
						app_stats.tx.ro_tx_failed_pkts);
	printf(" - Pkts transmitted w/o reorder:	%"PRIu64"\n",
						app_stats.tx.early_pkts_txtd_woro);
	printf(" - Pkts tx failed w/o reorder:		%"PRIu64"\n",
						app_stats.tx.early_pkts_tx_failed_woro);

	RTE_ETH_FOREACH_DEV(i) {
		rte_eth_stats_get(i, &eth_stats);
		printf("\nPort %u stats:\n", i);
		printf(" - Pkts in:   %"PRIu64"\n", eth_stats.ipackets);
		printf(" - Pkts out:  %"PRIu64"\n", eth_stats.opackets);
		printf(" - In Errs:   %"PRIu64"\n", eth_stats.ierrors);
		printf(" - Out Errs:  %"PRIu64"\n", eth_stats.oerrors);
		printf(" - Mbuf Errs: %"PRIu64"\n", eth_stats.rx_nombuf);
	}
}

static void
int_handler(int sig_num)
{
	printf("Exiting on signal %d\n", sig_num);
	quit_signal = 1;
}

/**
 * This thread receives mbufs from the port and affects them an internal
 * sequence number to keep track of their order of arrival through an
 * mbuf structure.
 * The mbufs are then passed to the worker threads via the rx_to_workers
 * ring.
 */
static int
rx_thread(struct rte_ring *ring_out)
{
	uint32_t seqn = 0;
	uint16_t i, ret = 0;
	uint16_t nb_rx_pkts;
	uint16_t port_id;
	struct rte_mbuf *pkts[MAX_PKTS_BURST];

	RTE_LOG(INFO, REORDERAPP, "%s() started on lcore %u\n", __func__,
							rte_lcore_id());

	while (!quit_signal) {

		RTE_ETH_FOREACH_DEV(port_id) {
			if ((portmask & (1 << port_id)) != 0) {

				/* receive packets */
				nb_rx_pkts = rte_eth_rx_burst(port_id, 0,
								pkts, MAX_PKTS_BURST);
				if (nb_rx_pkts == 0) {
					RTE_LOG_DP(DEBUG, REORDERAPP,
					"%s():Received zero packets\n",	__func__);
					continue;
				}
				app_stats.rx.rx_pkts += nb_rx_pkts;

				/* mark sequence number */
				for (i = 0; i < nb_rx_pkts; )
					pkts[i++]->seqn = seqn++;

				/* enqueue to rx_to_workers ring */
				ret = rte_ring_enqueue_burst(ring_out,
						(void *)pkts, nb_rx_pkts, NULL);
				app_stats.rx.enqueue_pkts += ret;
				if (unlikely(ret < nb_rx_pkts)) {
					app_stats.rx.enqueue_failed_pkts +=
									(nb_rx_pkts-ret);
					pktmbuf_free_bulk(&pkts[ret], nb_rx_pkts - ret);
				}
			}
		}
	}
	return 0;
}

/**
 * This thread takes bursts of packets from the rx_to_workers ring and
 * Changes the input port value to output port value. And feds it to
 * workers_to_tx
 */
static int
worker_thread(void *args_ptr)
{
	const uint16_t nb_ports = rte_eth_dev_count_avail();
	uint16_t i, ret = 0;
	uint16_t burst_size = 0;
	struct worker_thread_args *args;
	struct rte_mbuf *burst_buffer[MAX_PKTS_BURST] = { NULL };
	struct rte_ring *ring_in, *ring_out;
	const unsigned xor_val = (nb_ports > 1);

	args = (struct worker_thread_args *) args_ptr;
	ring_in  = args->ring_in;
	ring_out = args->ring_out;

	RTE_LOG(INFO, REORDERAPP, "%s() started on lcore %u\n", __func__,
							rte_lcore_id());

	while (!quit_signal) {

		/* dequeue the mbufs from rx_to_workers ring */
		burst_size = rte_ring_dequeue_burst(ring_in,
				(void *)burst_buffer, MAX_PKTS_BURST, NULL);
		if (unlikely(burst_size == 0))
			continue;

		__sync_fetch_and_add(&app_stats.wkr.dequeue_pkts, burst_size);

		/* just do some operation on mbuf */
		for (i = 0; i < burst_size;)
			burst_buffer[i++]->port ^= xor_val;

		/* enqueue the modified mbufs to workers_to_tx ring */
		ret = rte_ring_enqueue_burst(ring_out, (void *)burst_buffer,
				burst_size, NULL);
		__sync_fetch_and_add(&app_stats.wkr.enqueue_pkts, ret);
		if (unlikely(ret < burst_size)) {
			/* Return the mbufs to their respective pool, dropping packets */
			__sync_fetch_and_add(&app_stats.wkr.enqueue_failed_pkts,
					(int)burst_size - ret);
			pktmbuf_free_bulk(&burst_buffer[ret], burst_size - ret);
		}
	}
	return 0;
}

/**
 * Dequeue mbufs from the workers_to_tx ring and reorder them before
 * transmitting.
 */
static int
send_thread(struct send_thread_args *args)
{
	int ret;
	unsigned int i, dret;
	uint16_t nb_dq_mbufs;
	uint8_t outp;
	unsigned sent;
	struct rte_mbuf *mbufs[MAX_PKTS_BURST];
	struct rte_mbuf *rombufs[MAX_PKTS_BURST] = {NULL};
	static struct rte_eth_dev_tx_buffer *tx_buffer[RTE_MAX_ETHPORTS];

	RTE_LOG(INFO, REORDERAPP, "%s() started on lcore %u\n", __func__, rte_lcore_id());

	configure_tx_buffers(tx_buffer);

	while (!quit_signal) {

		/* deque the mbufs from workers_to_tx ring */
		nb_dq_mbufs = rte_ring_dequeue_burst(args->ring_in,
				(void *)mbufs, MAX_PKTS_BURST, NULL);

		if (unlikely(nb_dq_mbufs == 0))
			continue;

		app_stats.tx.dequeue_pkts += nb_dq_mbufs;

		for (i = 0; i < nb_dq_mbufs; i++) {
			/* send dequeued mbufs for reordering */
			ret = rte_reorder_insert(args->buffer, mbufs[i]);

			if (ret == -1 && rte_errno == ERANGE) {
				/* Too early pkts should be transmitted out directly */
				RTE_LOG_DP(DEBUG, REORDERAPP,
						"%s():Cannot reorder early packet "
						"direct enqueuing to TX\n", __func__);
				outp = mbufs[i]->port;
				if ((portmask & (1 << outp)) == 0) {
					rte_pktmbuf_free(mbufs[i]);
					continue;
				}
				if (rte_eth_tx_burst(outp, 0, (void *)mbufs[i], 1) != 1) {
					rte_pktmbuf_free(mbufs[i]);
					app_stats.tx.early_pkts_tx_failed_woro++;
				} else
					app_stats.tx.early_pkts_txtd_woro++;
			} else if (ret == -1 && rte_errno == ENOSPC) {
				/**
				 * Early pkts just outside of window should be dropped
				 */
				rte_pktmbuf_free(mbufs[i]);
			}
		}

		/*
		 * drain MAX_PKTS_BURST of reordered
		 * mbufs for transmit
		 */
		dret = rte_reorder_drain(args->buffer, rombufs, MAX_PKTS_BURST);
		for (i = 0; i < dret; i++) {

			struct rte_eth_dev_tx_buffer *outbuf;
			uint8_t outp1;

			outp1 = rombufs[i]->port;
			/* skip ports that are not enabled */
			if ((portmask & (1 << outp1)) == 0) {
				rte_pktmbuf_free(rombufs[i]);
				continue;
			}

			outbuf = tx_buffer[outp1];
			sent = rte_eth_tx_buffer(outp1, 0, outbuf, rombufs[i]);
			if (sent)
				app_stats.tx.ro_tx_pkts += sent;
		}
	}

	free_tx_buffers(tx_buffer);

	return 0;
}

/**
 * Dequeue mbufs from the workers_to_tx ring and transmit them
 */
static int
tx_thread(struct rte_ring *ring_in)
{
	uint32_t i, dqnum;
	uint8_t outp;
	unsigned sent;
	struct rte_mbuf *mbufs[MAX_PKTS_BURST];
	struct rte_eth_dev_tx_buffer *outbuf;
	static struct rte_eth_dev_tx_buffer *tx_buffer[RTE_MAX_ETHPORTS];

	RTE_LOG(INFO, REORDERAPP, "%s() started on lcore %u\n", __func__,
							rte_lcore_id());

	configure_tx_buffers(tx_buffer);

	while (!quit_signal) {

		/* deque the mbufs from workers_to_tx ring */
		dqnum = rte_ring_dequeue_burst(ring_in,
				(void *)mbufs, MAX_PKTS_BURST, NULL);

		if (unlikely(dqnum == 0))
			continue;

		app_stats.tx.dequeue_pkts += dqnum;

		for (i = 0; i < dqnum; i++) {
			outp = mbufs[i]->port;
			/* skip ports that are not enabled */
			if ((portmask & (1 << outp)) == 0) {
				rte_pktmbuf_free(mbufs[i]);
				continue;
			}

			outbuf = tx_buffer[outp];
			sent = rte_eth_tx_buffer(outp, 0, outbuf, mbufs[i]);
			if (sent)
				app_stats.tx.ro_tx_pkts += sent;
		}
	}

	return 0;
}

int
main(int argc, char **argv)
{
	int ret;
	unsigned nb_ports;
	unsigned int lcore_id, last_lcore_id, master_lcore_id;
	uint16_t port_id;
	uint16_t nb_ports_available;
	struct worker_thread_args worker_args = {NULL, NULL};
	struct send_thread_args send_args = {NULL, NULL};
	struct rte_ring *rx_to_workers;
	struct rte_ring *workers_to_tx;

	/* catch ctrl-c so we can print on exit */
	signal(SIGINT, int_handler);

	/* Initialize EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		return -1;

	argc -= ret;
	argv += ret;

	/* Parse the application specific arguments */
	ret = parse_args(argc, argv);
	if (ret < 0)
		return -1;

	/* Check if we have enought cores */
	if (rte_lcore_count() < 3)
		rte_exit(EXIT_FAILURE, "Error, This application needs at "
				"least 3 logical cores to run:\n"
				"1 lcore for packet RX\n"
				"1 lcore for packet TX\n"
				"and at least 1 lcore for worker threads\n");

	nb_ports = rte_eth_dev_count_avail();
	if (nb_ports == 0)
		rte_exit(EXIT_FAILURE, "Error: no ethernet ports detected\n");
	if (nb_ports != 1 && (nb_ports & 1))
		rte_exit(EXIT_FAILURE, "Error: number of ports must be even, except "
				"when using a single port\n");

	mbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", MBUF_PER_POOL,
			MBUF_POOL_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
			rte_socket_id());
	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "%s\n", rte_strerror(rte_errno));

	nb_ports_available = nb_ports;

	/* initialize all ports */
	RTE_ETH_FOREACH_DEV(port_id) {
		/* skip ports that are not enabled */
		if ((portmask & (1 << port_id)) == 0) {
			printf("\nSkipping disabled port %d\n", port_id);
			nb_ports_available--;
			continue;
		}
		/* init port */
		printf("Initializing port %u... done\n", port_id);

		if (configure_eth_port(port_id) != 0)
			rte_exit(EXIT_FAILURE, "Cannot initialize port %"PRIu8"\n",
					port_id);
	}

	if (!nb_ports_available) {
		rte_exit(EXIT_FAILURE,
			"All available ports are disabled. Please set portmask.\n");
	}

	/* Create rings for inter core communication */
	rx_to_workers = rte_ring_create("rx_to_workers", RING_SIZE, rte_socket_id(),
			RING_F_SP_ENQ);
	if (rx_to_workers == NULL)
		rte_exit(EXIT_FAILURE, "%s\n", rte_strerror(rte_errno));

	workers_to_tx = rte_ring_create("workers_to_tx", RING_SIZE, rte_socket_id(),
			RING_F_SC_DEQ);
	if (workers_to_tx == NULL)
		rte_exit(EXIT_FAILURE, "%s\n", rte_strerror(rte_errno));

	if (!disable_reorder) {
		send_args.buffer = rte_reorder_create("PKT_RO", rte_socket_id(),
				REORDER_BUFFER_SIZE);
		if (send_args.buffer == NULL)
			rte_exit(EXIT_FAILURE, "%s\n", rte_strerror(rte_errno));
	}

	last_lcore_id   = get_last_lcore_id();
	master_lcore_id = rte_get_master_lcore();

	worker_args.ring_in  = rx_to_workers;
	worker_args.ring_out = workers_to_tx;

	/* Start worker_thread() on all the available slave cores but the last 1 */
	for (lcore_id = 0; lcore_id <= get_previous_lcore_id(last_lcore_id); lcore_id++)
		if (rte_lcore_is_enabled(lcore_id) && lcore_id != master_lcore_id)
			rte_eal_remote_launch(worker_thread, (void *)&worker_args,
					lcore_id);

	if (disable_reorder) {
		/* Start tx_thread() on the last slave core */
		rte_eal_remote_launch((lcore_function_t *)tx_thread, workers_to_tx,
				last_lcore_id);
	} else {
		send_args.ring_in = workers_to_tx;
		/* Start send_thread() on the last slave core */
		rte_eal_remote_launch((lcore_function_t *)send_thread,
				(void *)&send_args, last_lcore_id);
	}

	/* Start rx_thread() on the master core */
	rx_thread(rx_to_workers);

	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			return -1;
	}

	print_stats();
	return 0;
}
