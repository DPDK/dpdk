/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include <stdint.h>
#include <inttypes.h>
#include <getopt.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_mbuf_dyn.h>

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

static int hwts_dynfield_offset = -1;

static inline rte_mbuf_timestamp_t *
hwts_field(struct rte_mbuf *mbuf)
{
	return RTE_MBUF_DYNFIELD(mbuf,
			hwts_dynfield_offset, rte_mbuf_timestamp_t *);
}

typedef uint64_t tsc_t;
static int tsc_dynfield_offset = -1;

static inline tsc_t *
tsc_field(struct rte_mbuf *mbuf)
{
	return RTE_MBUF_DYNFIELD(mbuf, tsc_dynfield_offset, tsc_t *);
}

static const char usage[] =
	"%s EAL_ARGS -- [-t]\n";

static const struct rte_eth_conf port_conf_default = {
	.rxmode = {
		.max_rx_pkt_len = RTE_ETHER_MAX_LEN,
	},
};

static struct {
	uint64_t total_cycles;
	uint64_t total_queue_cycles;
	uint64_t total_pkts;
} latency_numbers;

int hw_timestamping;

#define TICKS_PER_CYCLE_SHIFT 16
static uint64_t ticks_per_cycle_mult;

/* Callback added to the RX port and applied to packets. 8< */
static uint16_t
add_timestamps(uint16_t port __rte_unused, uint16_t qidx __rte_unused,
		struct rte_mbuf **pkts, uint16_t nb_pkts,
		uint16_t max_pkts __rte_unused, void *_ __rte_unused)
{
	unsigned i;
	uint64_t now = rte_rdtsc();

	for (i = 0; i < nb_pkts; i++)
		*tsc_field(pkts[i]) = now;
	return nb_pkts;
}
/* >8 End of callback addition and application. */

/* Callback is added to the TX port. 8< */
static uint16_t
calc_latency(uint16_t port, uint16_t qidx __rte_unused,
		struct rte_mbuf **pkts, uint16_t nb_pkts, void *_ __rte_unused)
{
	uint64_t cycles = 0;
	uint64_t queue_ticks = 0;
	uint64_t now = rte_rdtsc();
	uint64_t ticks;
	unsigned i;

	if (hw_timestamping)
		rte_eth_read_clock(port, &ticks);

	for (i = 0; i < nb_pkts; i++) {
		cycles += now - *tsc_field(pkts[i]);
		if (hw_timestamping)
			queue_ticks += ticks - *hwts_field(pkts[i]);
	}

	latency_numbers.total_cycles += cycles;
	if (hw_timestamping)
		latency_numbers.total_queue_cycles += (queue_ticks
			* ticks_per_cycle_mult) >> TICKS_PER_CYCLE_SHIFT;

	latency_numbers.total_pkts += nb_pkts;

	if (latency_numbers.total_pkts > (100 * 1000 * 1000ULL)) {
		printf("Latency = %"PRIu64" cycles\n",
		latency_numbers.total_cycles / latency_numbers.total_pkts);
		if (hw_timestamping) {
			printf("Latency from HW = %"PRIu64" cycles\n",
			   latency_numbers.total_queue_cycles
			   / latency_numbers.total_pkts);
		}
		latency_numbers.total_cycles = 0;
		latency_numbers.total_queue_cycles = 0;
		latency_numbers.total_pkts = 0;
	}
	return nb_pkts;
}
/* >8 End of callback addition. */

/*
 * Initialises a given port using global settings and with the rx buffers
 * coming from the mbuf_pool passed as parameter
 */

 /* Port initialization. 8< */
static inline int
port_init(uint16_t port, struct rte_mempool *mbuf_pool)
{
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rx_rings = 1, tx_rings = 1;
	uint16_t nb_rxd = RX_RING_SIZE;
	uint16_t nb_txd = TX_RING_SIZE;
	int retval;
	uint16_t q;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_rxconf rxconf;
	struct rte_eth_txconf txconf;

	if (!rte_eth_dev_is_valid_port(port))
		return -1;

	retval = rte_eth_dev_info_get(port, &dev_info);
	if (retval != 0) {
		printf("Error during getting device (port %u) info: %s\n",
				port, strerror(-retval));

		return retval;
	}

	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
		port_conf.txmode.offloads |=
			DEV_TX_OFFLOAD_MBUF_FAST_FREE;

	if (hw_timestamping) {
		if (!(dev_info.rx_offload_capa & DEV_RX_OFFLOAD_TIMESTAMP)) {
			printf("\nERROR: Port %u does not support hardware timestamping\n"
					, port);
			return -1;
		}
		port_conf.rxmode.offloads |= DEV_RX_OFFLOAD_TIMESTAMP;
		rte_mbuf_dyn_rx_timestamp_register(&hwts_dynfield_offset, NULL);
		if (hwts_dynfield_offset < 0) {
			printf("ERROR: Failed to register timestamp field\n");
			return -rte_errno;
		}
	}

	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
		return retval;

	retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
	if (retval != 0)
		return retval;

	rxconf = dev_info.default_rxconf;

	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, nb_rxd,
			rte_eth_dev_socket_id(port), &rxconf, mbuf_pool);
		if (retval < 0)
			return retval;
	}

	txconf = dev_info.default_txconf;
	txconf.offloads = port_conf.txmode.offloads;
	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, nb_txd,
				rte_eth_dev_socket_id(port), &txconf);
		if (retval < 0)
			return retval;
	}

	retval  = rte_eth_dev_start(port);
	if (retval < 0)
		return retval;

	if (hw_timestamping && ticks_per_cycle_mult  == 0) {
		uint64_t cycles_base = rte_rdtsc();
		uint64_t ticks_base;
		retval = rte_eth_read_clock(port, &ticks_base);
		if (retval != 0)
			return retval;
		rte_delay_ms(100);
		uint64_t cycles = rte_rdtsc();
		uint64_t ticks;
		rte_eth_read_clock(port, &ticks);
		uint64_t c_freq = cycles - cycles_base;
		uint64_t t_freq = ticks - ticks_base;
		double freq_mult = (double)c_freq / t_freq;
		printf("TSC Freq ~= %" PRIu64
				"\nHW Freq ~= %" PRIu64
				"\nRatio : %f\n",
				c_freq * 10, t_freq * 10, freq_mult);
		/* TSC will be faster than internal ticks so freq_mult is > 0
		 * We convert the multiplication to an integer shift & mult
		 */
		ticks_per_cycle_mult = (1 << TICKS_PER_CYCLE_SHIFT) / freq_mult;
	}

	struct rte_ether_addr addr;

	retval = rte_eth_macaddr_get(port, &addr);
	if (retval < 0) {
		printf("Failed to get MAC address on port %u: %s\n",
			port, rte_strerror(-retval));
		return retval;
	}
	printf("Port %u MAC: %02"PRIx8" %02"PRIx8" %02"PRIx8
			" %02"PRIx8" %02"PRIx8" %02"PRIx8"\n",
			(unsigned)port,
			RTE_ETHER_ADDR_BYTES(&addr));

	retval = rte_eth_promiscuous_enable(port);
	if (retval != 0)
		return retval;

	/* RX and TX callbacks are added to the ports. 8< */
	rte_eth_add_rx_callback(port, 0, add_timestamps, NULL);
	rte_eth_add_tx_callback(port, 0, calc_latency, NULL);
	/* >8 End of RX and TX callbacks. */

	return 0;
}
/* >8 End of port initialization. */

/*
 * Main thread that does the work, reading from INPUT_PORT
 * and writing to OUTPUT_PORT
 */
static  __rte_noreturn void
lcore_main(void)
{
	uint16_t port;

	RTE_ETH_FOREACH_DEV(port)
		if (rte_eth_dev_socket_id(port) > 0 &&
				rte_eth_dev_socket_id(port) !=
						(int)rte_socket_id())
			printf("WARNING, port %u is on remote NUMA node to "
					"polling thread.\n\tPerformance will "
					"not be optimal.\n", port);

	printf("\nCore %u forwarding packets. [Ctrl+C to quit]\n",
			rte_lcore_id());
	for (;;) {
		RTE_ETH_FOREACH_DEV(port) {
			struct rte_mbuf *bufs[BURST_SIZE];
			const uint16_t nb_rx = rte_eth_rx_burst(port, 0,
					bufs, BURST_SIZE);
			if (unlikely(nb_rx == 0))
				continue;
			const uint16_t nb_tx = rte_eth_tx_burst(port ^ 1, 0,
					bufs, nb_rx);
			if (unlikely(nb_tx < nb_rx)) {
				uint16_t buf;

				for (buf = nb_tx; buf < nb_rx; buf++)
					rte_pktmbuf_free(bufs[buf]);
			}
		}
	}
}

/* Main function, does initialisation and calls the per-lcore functions */
int
main(int argc, char *argv[])
{
	struct rte_mempool *mbuf_pool;
	uint16_t nb_ports;
	uint16_t portid;
	struct option lgopts[] = {
		{ NULL,  0, 0, 0 }
	};
	int opt, option_index;

	static const struct rte_mbuf_dynfield tsc_dynfield_desc = {
		.name = "example_bbdev_dynfield_tsc",
		.size = sizeof(tsc_t),
		.align = __alignof__(tsc_t),
	};

	/* init EAL */
	int ret = rte_eal_init(argc, argv);

	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
	argc -= ret;
	argv += ret;

	while ((opt = getopt_long(argc, argv, "t", lgopts, &option_index))
			!= EOF)
		switch (opt) {
		case 't':
			hw_timestamping = 1;
			break;
		default:
			printf(usage, argv[0]);
			return -1;
		}
	optind = 1; /* reset getopt lib */

	nb_ports = rte_eth_dev_count_avail();
	if (nb_ports < 2 || (nb_ports & 1))
		rte_exit(EXIT_FAILURE, "Error: number of ports must be even\n");

	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL",
		NUM_MBUFS * nb_ports, MBUF_CACHE_SIZE, 0,
		RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	tsc_dynfield_offset =
		rte_mbuf_dynfield_register(&tsc_dynfield_desc);
	if (tsc_dynfield_offset < 0)
		rte_exit(EXIT_FAILURE, "Cannot register mbuf field\n");

	/* initialize all ports */
	RTE_ETH_FOREACH_DEV(portid)
		if (port_init(portid, mbuf_pool) != 0)
			rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu16"\n",
					portid);

	if (rte_lcore_count() > 1)
		printf("\nWARNING: Too much enabled lcores - "
			"App uses only 1 lcore\n");

	/* call lcore_main on main core only */
	lcore_main();

	/* clean up the EAL */
	rte_eal_cleanup();

	return 0;
}
